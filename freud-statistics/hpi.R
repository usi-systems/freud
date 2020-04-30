get_hpi <- function(vec) {
	library(ks)
	
	res <- hscv(vec)
	return(res)
}

get_cc <- function(mat) {
	pcc <- max(abs(cor(x=mat[,1], y=mat[,2], method="pearson")), abs(cor(x=mat[,1], y=mat[,2], method="spearman")))
	return(pcc)
}

check_model <- function(coeff, data, nlogn) {
	x <- data[2,]
	y_actual <- data[1,]
	if (nlogn) {
		mod <- lm(formula = as.formula(y_actual ~ do.call("offset", list(coeff[2] * x)) + do.call("offset", list(coeff[3] * x * log2(x)))))
	} else {
		mod <- lm(formula = as.formula(y_actual ~ do.call("offset", list(coeff[2] * x)) + do.call("offset", list(coeff[3] * x**2))))
	}
	mod$coefficients[1] = coeff[1] # set the intercept
	xdf <- data.frame(x = x)
	y_pred <- predict(mod, xdf)
	rss <- sum((y_pred - y_actual) ^ 2)  ## residual sum of squares
	tss <- sum((y_actual - mean(y_actual)) ^ 2)  ## total sum of squares
	rsq <- 1 - rss/tss
	return(rsq)
}

cor_wrap <- function(mat) {
	res <- cor(mat)
	return(res)
}

get_multimodel <- function(mat, frm) {
	#print(frm)
	tryCatch({
		mod <- lm(eval(parse(text=frm)))
		sum <- summary(mod)
		bic <- BIC(mod)
		numterms <- mod$rank
		res <- c(bic, sum$r.squared, mod$coefficients, sum$coefficients[(numterms)*3+1:numterms])
		return(res)
	},
		# Not possible to compute the lm, return 0
		error = function(e) { return(0) }
	)
}

get_best_model <- function(mat) {
	bic_threshold <- 15
	best_is_nlogn <- 0
	y <- mat[1,]
	x <- mat[2,]
	
	# Linear model
	model <- lm(y ~ x)
	linear_bic <- BIC(model)
	best_model <- model

	# Nlogn model
	xlogx <- x * log2(x)
	model <- lm(y ~ x + xlogx)
	nlogn_bic <- BIC(model)
	if (nlogn_bic < linear_bic - bic_threshold) {
		best_model <- model
		best_is_nlogn <- 1
	}

	# Quadratic model
	x2 <- x ** 2
	model <- lm(y ~ x + x2)
	quad_bic <- BIC(model)
	if (quad_bic < linear_bic - bic_threshold) {
		if (!best_is_nlogn || quad_bic < nlogn_bic) {
			best_model <- model
			best_is_nlogn <- 0
		}
	}

	res <- c(summary(best_model)$r.squared, best_model$rank + best_is_nlogn)
	for (i in 1:best_model$rank) {
		res <- append(res, best_model$coefficients[i])
	}

	return(res)

}


####################
# Local minima and maxima: https://stackoverflow.com/questions/6836409/finding-local-maxima-and-minima
# Modified to account for the smoothness factor
localMaxima <- function(x) {
  smooth_factor <- 0L #0.000003
  # Use -Inf instead if x is numeric (non-integer)
  y <- diff(c(-Inf, x)) > smooth_factor 
  rle(y)$lengths
  y <- cumsum(rle(y)$lengths)
  y <- y[seq.int(1L, length(y), 2L)]
  if (x[[1]] == x[[2]]) {
    y <- y[-1]
  }
  y
}

localMinima <- function(x) {
  # Use -Inf instead if x is numeric (non-integer)
  smooth_factor <- 0L #0.000003
  y <- diff(c(Inf, x)) > smooth_factor
  rle(y)$lengths
  y <- cumsum(rle(y)$lengths)
  y <- y[seq.int(1L, length(y), 2L)]
  if (x[[1]] == x[[2]]) {
    y <- y[-1]
  }
  y
}
####################
### VARIABLE KDE ###
#' Broadcasting an array to a wanted shape
#'
#' Extending a given array to match a wanted shape, similar to repmat.
#' @param arr Array to be broadcasted
#' @param dims Desired output dimensions
#'
#' @return array of dimension dim
#' @examples
#'   M = array(1:12,dim=c(1,2,3))
#'   N = broadcast(M,c(4,2,3))
#'
#' @note # Fri Feb  9 14:41:25 2018 ------------------------------
#' @author Feng Geng (shouldsee.gem@gmail.com)
#' @export
broadcast  <- function(arr, dims){
  DIM = dim(arr)
  EXCLUDE = which(DIM==dims) #### find out the margin that will be excluded
  if (length(EXCLUDE)!=0){
    if( !all(DIM[-EXCLUDE]==1) ){
      errmsg = sprintf("All non-singleton dimensions must be equal: Array 1 [%s] , Wanted shape: [%s]",
                       paste(DIM,collapse = ','),
                       paste(dims,collapse = ','))
      stop(errmsg)
    }
    perm <- c(EXCLUDE, seq_along(dims)[-EXCLUDE])
  }else{
    perm <- c(seq_along(dims))
  }

  arr = array(aperm(arr, perm), dims[perm])
  arr = aperm(arr, order(perm)
              ,resize = T)
}
#' Broadcasting an operation on multi-dimensinoal arrays
#'
#' Broadcasting two arrays to the same shape and then apply FUN.
#'
#' @param arrA,arrB Arrays to perfom the operator FUN(arrA,arrB)
#' @param FUN function to be performed (must be vectorised)
#'
#' @return An array of the shape with each dimension being the maximum of dimA,dimB.
#' @examples
#'
#'   M = array(1:12,dim=c(2,2,3))
#'   N = rbind(c(1,2))
#'   O = bsxfun(M,N,'*')
#' @note
#'
#' # Fri Feb  9 14:41:25 2018 ------------------------------
#'
#' @author Feng Geng (shouldsee.gem@gmail.com)
#' @export
#'
# Thanks to https://github.com/shouldsee/Rutil/blob/master/R/bsxfun.R 
bsxfun <-  function(arrA,arrB,FUN = '+',...){
   FUN <- match.fun(FUN)
   arrA <- as.array(arrA)
   arrB <- as.array(arrB)
   dimA <- dim(arrA)
   dimB <- dim(arrB)
   if (identical(dimA,dimB)){
     #### Trivial scenario
     arr = FUN(arrA,arrB, ...)
     return(arr)
   }

   arrL = list(arrA,arrB)
   orient <- order(sapply(list(dimA,dimB),length))
   # orient <- order(sapply(arrL,length))
   arrL = arrL[orient]
   dim1 <- dim(arrL[[1]])
   dim2 <- dim(arrL[[2]])
   dim1 <- c(dim1, rep(1,length(dim2)-length(dim1))) #### Padding smaller array to be of same length
   dim(arrL[[1]])<-dim1;
   nonS = (dim1!=1) & (dim2!=1)

   if( !all(dim1[nonS]== dim2[nonS])){
     errmsg = sprintf("All non-singleton dimensions must be equal: Array 1 [%s] , Array 2 [%s]",
                      paste(dimA,collapse = ','),
                      paste(dimB,collapse = ','))
     stop(errmsg)
   }
   ### Broadcasting to fill singleton dimensions
   # browser()
   dims  <- pmax(dim1,dim2)
   arrL = lapply(arrL,function(x)broadcast(x,dims))
   arrL = arrL[order(orient)]
   arr = FUN(arrL[[1]],arrL[[2]], ...)
   return(arr)
}

if (interactive()){

  M = array(1:12,dim=c(2,2,3))
  N = rbind(c(1,2))
  O = bsxfun(M,N,'*')

  M = array(1:12,dim=c(2,2,3))
  N = rbind(c(1,2))
  O = bsxfun(N,M,'*')
}

#  Reference:
#  Kernel density estimation via diffusion
#  Z. I. Botev, J. F. Grotowski, and D. P. Kroese (2010)
#  Annals of Statistics, Volume 38, Number 5, pages 2916-2957.
akde1d <- function(X,grid,gam) {
	# begin scaling preprocessing
	n = NROW(X);d = NCOL(X);
	MAX = max(X);MIN = min(X);scaling=MAX-MIN;
	MAX=MAX+scaling/10;MIN=MIN-scaling/10;scaling=MAX-MIN;
	X=bsxfun(X,MIN,'-');X=bsxfun(X,scaling,'/');
	#if (nargin<2)|isempty(grid) % failing to provide grid
	grid=seq(MIN,MAX,scaling/(2^12-1));
	#end
	# FIXME: check @rdivide matlab operation
	mesh=bsxfun(grid,MIN,'-');mesh=bsxfun(mesh,scaling,'/');
	#if nargin<3 % failing to provide speed/accuracy tradeoff
	gam=ceiling(n^(1/3))+20;
	if (gam > n) {
		gam = gam - 20
	}
	#end
	#end preprocessing
	# algorithm initialization
	print(n)
	print(gam)
	del=.2/n^(d/(d+4));perm=sample(n);mu=matrix(X[perm[1:gam]], gam, d);
	w=matrix(runif(gam), 1, gam);w=w/sum(w);Sig=(del^2)*matrix(runif(gam),gam,d);
	
	ent=-Inf;
	for (iter in seq(1,1500)) {
	  Eold = ent;
	  rEM=regEM(w,mu,Sig,del,X); # update parameters
	  w=rEM$w;mu=rEM$mu;Sig=rEM$sig;del=rEM$del;ent=rEM$ent;
	  err=abs((ent-Eold)/ent); # stopping condition
	  #print(ent)
	  #print(Eold)
	  #print('Iter.    Tol.      Bandwidth ');
	  #print('%4i    %8.2e   %8.2e\n',iter,err,del);
	  #fprintf('----------------------------\n');
	  #print(ent)
	  #print(Eold)
	  #print(err)
	  if (iter > 200) {
	    break #, end
	  }
	  if (err < 0.00001) {
	    break #, end
	  }
	}
	pdf = probfun(mesh,w,mu,Sig)/prod(scaling); # evaluate density
	del=del*scaling; # adjust bandwidth for scaling
	return(list("pdf"=pdf, "grid"=grid));
}

####################################################
probfun <- function(x,w,mu,Sig) {
  gam=NROW(mu);
  d=NCOL(mu);
  out=0;
  for (k in seq(1,gam)) {
      S=Sig[k,];
      xx=bsxfun(x,mu[k,],'-');
      xx=bsxfun(xx^2,S,'/'); 
      out=out+exp(-.5*matrix(apply(xx, 1, function(x) sum(x)), NROW(xx), 1)+log(w[k])-.5*sum(log(S))-d*log(2*pi)/2);
  }
  return(out);
}
####################################################
regEM <- function(w,mu,Sig,del,X) { 
  #function [w,mu,Sig,del,ent]=regEM(w,mu,Sig,del,X)
  gam=NROW(mu);d=NCOL(mu);
  n=NROW(X);d=NCOL(X);
  log_lh=matrix(0,n,gam); log_sig=log_lh;
  #print(gam)
  #print(w)
  #print(mu)
  for (i in seq(1,gam)) {
    #print(i)
    #print(Sig)
    s=Sig[i,];
    Xcentered = bsxfun(X, mu[i],'-');
    xRinv = bsxfun(Xcentered^2, s,'/');
    tmp = bsxfun(xRinv, s,'/')
    xSig = matrix(apply(tmp, 1, function(x) sum(x)), NROW(tmp), 1)+.Machine$double.eps;
    log_lh[,i]=-.5*matrix(apply(xRinv, 1, function(x) sum(x)), NROW(xRinv), 1)-.5*sum(log(s))+log(w[i])-d*log(2*pi)/2-.5*del^2*sum(1./s);
    log_sig[,i]=log_lh[,i]+log(xSig);
  }
  maxll = matrix(apply(log_lh, 1, function(x) max(x)), NROW(log_lh), 1); 
  maxlsig = matrix(apply(log_sig, 1, function(x) max(x)), NROW(log_sig), 1);
  p=exp(bsxfun(log_lh, maxll, '-'));
  #print(log_lh)
  psig=exp(bsxfun(log_sig, maxlsig, '-'));
  density=matrix(apply(p, 1, function(x) sum(x)), NROW(p), 1);  # density = sum(p,2); 
  psigd=matrix(apply(psig, 1, function(x) sum(x)), NROW(psig), 1); # psigd=sum(psig,2);
  logpdf=log(density)+maxll; logpsigd=log(psigd)+maxlsig;
  p = bsxfun(p, density,'/'); #% normalize classification prob.
  ent=sum(logpdf); 
  w = apply(p, 2, function(x) sum(x)); # w=sum(p,1);
  #print(which(w>0))
  for (i in which(w>0)) {
    mu[i]=t(p[,i]) %*% X / w[i];  # compute mu's
    Xcentered = bsxfun(X,mu[i],'-');
    Sig[i,]=t(p[,i]) %*% (Xcentered^2)/w[i]+del^2; # compute sigmas
  }
  #print(Sig)
  w=w/sum(w);
  curv=mean(exp(logpsigd-logpdf));
  del=1/(4*n*(4*pi)^(d/2)*curv)^(1/(d+2));
  return(list("w" = w, "mu" = mu, "sig" = Sig, "del" = del, "ent" = ent));
}


####################

kde_clustering <- function(vec) {
	library(ks)
	k <- kde(vec, positive = T)
	maxima <- k$eval.points[localMaxima(k$estimate)]	
	minima <- k$eval.points[localMinima(k$estimate)]
	res <- c(as.numeric(length(maxima)))
	res <- append(res, maxima)
	res <- append(res, as.numeric(length(minima)))
	res <- append(res, minima)
	return(res)		
}

vkde_clustering <- function(vec) {
	k <- akde1d(vec) 
	maxima <- k$grid[localMaxima(k$pdf)]	
	minima <- k$grid[localMinima(k$pdf)]
	res <- c(as.numeric(length(maxima)))
	res <- append(res, maxima)
	res <- append(res, as.numeric(length(minima)))
	res <- append(res, minima)
	return(res)		
}
